 package io.github.usr40k.esp32tankrobot.views;

 import android.content.Context;
 import android.graphics.Canvas;
 import android.util.AttributeSet;
 import android.view.MotionEvent;
 import android.widget.SeekBar;

 public class VerticalSeekBar extends SeekBar {

     public VerticalSeekBar(Context context) {
         super(context);
     }

     public VerticalSeekBar(Context context, AttributeSet attrs) {
         super(context, attrs);
     }

     public VerticalSeekBar(Context context, AttributeSet attrs, int defStyle) {
         super(context, attrs, defStyle);
     }

     @Override
     protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
         // Swap width and height for vertical orientation
         super.onMeasure(heightMeasureSpec, widthMeasureSpec);
         setMeasuredDimension(getMeasuredHeight(), getMeasuredWidth());
     }

     @Override
     protected void onDraw(Canvas c) {
         c.rotate(-90);
         c.translate(-getHeight(), 0);
         super.onDraw(c);
     }

     @Override
     public boolean onTouchEvent(MotionEvent event) {
         if (!isEnabled()) {
             return false;
         }

         switch (event.getAction()) {
             case MotionEvent.ACTION_DOWN:
             case MotionEvent.ACTION_MOVE:
             case MotionEvent.ACTION_UP:
                 int height = getHeight();
                 int y = (int) event.getY();
                 // Convert touch position to progress (0-400)
                 int progress = (int) (getMax() * y / height);
                 setProgress(progress);
                 onSizeChanged(getWidth(), getHeight(), 0, 0);
                 return true;

             default:
                 return false;
         }
     }
 }
